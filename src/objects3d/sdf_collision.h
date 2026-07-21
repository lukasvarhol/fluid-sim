#pragma once
#include "editor_state.h"
#include "../app/app_state.h"
#include "../linear_algebra.h"
#include "particle_config.h"
#include <array>
#include <cmath>
#include <numeric>

#ifdef USE_CUDA
#include "../cuda_buffers.cuh"
#endif

void addCollider(SDFCollider *colliders, RGObject *objects, size_t idx,
                 AppState *as);

void deleteCollider(SDFCollider* colliders, size_t idx, AppState* as);

void BuildSDFColliders(const std::vector<RGObject>& objects, std::vector<SDFCollider>& colliders);

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
inline float sdfCappedCylinder(Vec3 p, float radius, float halfHeight) {
  Vec2 d{ Vec2{p.x, p.y}.Magnitude() - radius,
          abs(p.z) - halfHeight };
  Vec2 dPos{ max(d.x, 0.0f), max(d.y, 0.0f) };
  return min(max(d.x, d.y), 0.0f) + dPos.Magnitude();
}

DEVICE_CALLABLE
inline float sdfTorus(Vec3 p, Vec2 t) {
  Vec2 q = Vec2{Vec2{p.x, p.z}.Magnitude() - t.x, p.y};
  return q.Magnitude() - t.y;
}

DEVICE_CALLABLE
inline float sdfTorusX(Vec3 p, Vec2 t) {
  Vec2 q = Vec2{Vec2{p.y, p.z}.Magnitude() - t.x, p.x};
  return q.Magnitude() - t.y;
}

DEVICE_CALLABLE
inline float sdfSphere(Vec3 p, float r) { return p.Magnitude() - r; }

DEVICE_CALLABLE
inline float sdfWedgeShell(Vec3 p, float outerRadius, float innerRadius, float wedgeAngle) {
  float outer = p.Magnitude() - outerRadius;
  float inner = p.Magnitude() - innerRadius;
  float shell = max(outer, -inner);
  
  float halfAngle = wedgeAngle * 0.5f;
  float s = std::sin(halfAngle);
  float c = std::cos(halfAngle);
  
  Vec3 n1{0.0f, 0.0f, -1.0};
  Vec3 n2{0.0f, 0.8f, 0.6f};
  
  return max(max(shell, -p.Dot(n1)), -p.Dot(n2));
}

DEVICE_CALLABLE
inline float sdfTorusWedgeShell(Vec3 p, float majorRadius, float outerRadius, float innerRadius, float wedgeAngle) {
  float outer = sdfTorusX(p, Vec2{majorRadius, outerRadius});
  float inner = sdfTorusX(p, Vec2{majorRadius, innerRadius});
  float shell = max(outer, -inner);

  Vec3 n1{0.0f, 0.0f, -1.0};
  Vec3 n2{0.0f, 0.8f, 0.6f};

  float cut = max(shell, Vec2{p.y, p.z}.Magnitude() - majorRadius);
  return max(max(cut, -p.Dot(n1) ), -p.Dot(n2));
}

DEVICE_CALLABLE
inline float sdfLChannel(Vec3 p) {
  constexpr float majorRadius = 0.2f;
  constexpr float outerRadius = 0.2f;
  constexpr float wall = 0.08f;
  constexpr float innerRadius = outerRadius - wall;

  Vec3 pTorus = p - Vec3{-0.2f, 0.0f, 0.2f};
  float outer = sdfTorus(pTorus, Vec2{majorRadius, outerRadius});
  float inner = sdfTorus(pTorus, Vec2{majorRadius, innerRadius});
  float shell = max(outer, -inner);

  float quarter = max(max(shell, -pTorus.x ), pTorus.z);
  return max(quarter, pTorus.y);
}

DEVICE_CALLABLE
inline float sdfSChannel(Vec3 p) {
  constexpr float outerRadius = 0.2f;
  constexpr float wall = 0.08f;
  constexpr float halfLength = 0.205f;
  constexpr float innerRadius = outerRadius - wall;

  float outer = sdfCappedCylinder(p, outerRadius, halfLength);
  float inner = sdfCappedCylinder(p, innerRadius, halfLength + 0.001f);
  float shell = max(outer, -inner);
  return max(shell, p.y);
}

DEVICE_CALLABLE
inline float sdfBRamp(Vec3 p) {
  constexpr float c = 0.6f;
  constexpr float s = 0.8f;
  constexpr float outerRadius = 0.193f;
  constexpr float majorRadius = 0.193f;
  constexpr float wall = 0.08f;
  constexpr float innerRadius = outerRadius - wall;

  p.z = -p.z;
  Vec3 pLocal = p - Vec3{0.0f, 0.157f, 0.09f};
  Vec3 pRotated{pLocal.x, pLocal.y * c + pLocal.z * s,
                -pLocal.y * s + pLocal.z * c};
  float mid = sdfSChannel(pRotated);

  // Wedge 1 at (0, 0.2, -0.2)
  Vec3 pS1 = p - Vec3{0.0f, 0.0f, 0.21f};
  pS1.y = -pS1.y;
  float s1 = sdfWedgeShell(pS1, outerRadius, innerRadius, 53.1f * PI / 180.0f);
  
  Vec3 pS2 = p - Vec3{0.0f, 0.20f, -0.2f};
  pS2.z = -pS2.z;
  float s2 = sdfTorusWedgeShell(pS2, majorRadius, outerRadius, innerRadius,  53.1f * PI / 180.0f);

  return min(min(mid, s1), s2);
}


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

DEVICE_CALLABLE
inline Vec3 sdfGradient(RGObjectType type, Vec3 localPosition) {
  const float epsilon = 0.001f;
  Vec3 dx{epsilon, 0, 0};
  Vec3 dy{0, epsilon, 0};
  Vec3 dz{0, 0, epsilon};

  Vec3 grad{
    sdfDispatch(type, localPosition + dx) - sdfDispatch(type, localPosition - dx),
    sdfDispatch(type, localPosition + dy) - sdfDispatch(type, localPosition - dy),
    sdfDispatch(type, localPosition + dz) - sdfDispatch(type, localPosition - dz)
  };
  return Normalize(grad);
}

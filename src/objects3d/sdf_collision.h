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
float sdfDispatch(RGObjectType type, Vec3 localPosition);
Vec3 sdfGradient(RGObjectType type, Vec3 localPosition);
void ProjectParticleSDF(Vec3& position, Vec3& velocity, const SDFCollider& collider);

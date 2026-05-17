#include "sdf_collision.h"

void BuildSDFColliders(const std::vector<RGObject> &objects,
                  std::vector<SDFCollider> &colliders) {
  colliders.clear();
  for (const auto &object : objects) {
    if (!object.active)
      continue; // exclude preview objects from collisions
    if (object.type == RGObjectType::BOX) continue;

    Mat4 rotationMatrix = CreateMatrixRotationXYZ(object.rotation);
    std::array<Vec3, 3> axes = {TransformDir(rotationMatrix, {1, 0, 0}),
                                   TransformDir(rotationMatrix, {0, 1, 0}),
                                   TransformDir(rotationMatrix, {0, 0, 1})};

    SDFCollider collider;
    collider.type = object.type;
    collider.worldPosition = object.position;
    collider.rotationAxes = axes;
    collider.restitution = energyRetention;

    colliders.push_back(collider);
  }
}

float sdfCappedCylinder(Vec3 p, float radius, float halfHeight) {
  Vec2 d{ Vec2{p.x, p.y}.Magnitude() - radius,
          std::abs(p.z) - halfHeight };
  Vec2 dPos{ std::max(d.x, 0.0f), std::max(d.y, 0.0f) };
  return std::min(std::max(d.x, d.y), 0.0f) + dPos.Magnitude();
}

float sdfSChannel(Vec3 p) {
  const float outerRadius = 0.2f;
  const float wall = 0.05;
  const float halfLength = 0.2;
  const float innerRadius = outerRadius - wall;

  float outer = sdfCappedCylinder(p, outerRadius, halfLength);
  float inner = sdfCappedCylinder(p, innerRadius, halfLength + 0.001f);
  float shell = std::max(outer, -inner);
  return std::max(shell, p.y);
}

float sdfDispatch(RGObjectType type, Vec3 localPosition) {
  switch (type) {
  case RGObjectType::S_CHANNEL:
    return sdfSChannel(localPosition);
  default:
    return 1e9f; // too far away
  }
}

Vec3 sdfGradient(RGObjectType type, Vec3 localPosition) {
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

void ProjectParticleSDF(Vec3& position, Vec3& velocity, const SDFCollider &collider) {
  Vec3 d = position - collider.worldPosition;
  Vec3 localPosition = {
    d.Dot(collider.rotationAxes[0]),
    d.Dot(collider.rotationAxes[1]),
    d.Dot(collider.rotationAxes[2])
  };

  float pushDistance = sdfDispatch(collider.type, localPosition);
  if (pushDistance >= 0.0f)
    return;

  Vec3 localGradient = sdfGradient(collider.type, localPosition);
  Vec3 worldGradient = collider.rotationAxes[0] * localGradient.x +
                       collider.rotationAxes[1] * localGradient.y +
                       collider.rotationAxes[2] * localGradient.z;
  position += worldGradient * (-pushDistance + 0.002f);
  float velocityDot = velocity.Dot(worldGradient);
  if (velocityDot < 0.0f)
    velocity -= worldGradient * ((1.0f + collider.restitution) * velocityDot);
}

#include "sdf_collision.h"
#include <cstring>

void addCollider(SDFCollider* colliders, RGObject* objects, size_t idx) {
  if (idx > MAX_OBJECTS-1)
    return;
  RGObject rgObject = objects[idx];
  Mat4 rotationMatrix = CreateMatrixRotationXYZ(rgObject.rotation);
  Vec3 axes[3] = {TransformDir(rotationMatrix, {1, 0, 0}),
                  TransformDir(rotationMatrix, {0, 1, 0}),
                  TransformDir(rotationMatrix, {0, 0, 1})};

  SDFCollider collider;
  collider.type = rgObject.type;
  collider.worldPosition = rgObject.position;
  memcpy(collider.rotationAxes, axes, sizeof(axes));
  collider.restitution = energyRetention;

  colliders[idx] = collider;
}

void deleteCollider(SDFCollider *colliders, RGObject* objects, size_t idx) {
  if (idx > MAX_OBJECTS - 1)
    return;
  SDFCollider collider;
  collider.type = RGObjectType::BOX;
  colliders[idx] = collider;
}

void BuildSDFColliders(const std::vector<RGObject> &objects,
                  std::vector<SDFCollider> &colliders) {
  colliders.clear();
  for (const auto &object : objects) {
    if (!object.active)
      continue; // exclude preview objects from collisions
    if (object.type == RGObjectType::BOX) continue;

    Mat4 rotationMatrix = CreateMatrixRotationXYZ(object.rotation);
    Vec3 axes[3] = {TransformDir(rotationMatrix, {1, 0, 0}),
                                   TransformDir(rotationMatrix, {0, 1, 0}),
                                   TransformDir(rotationMatrix, {0, 0, 1})};

    SDFCollider collider;
    collider.type = object.type;
    collider.worldPosition = object.position;
    memcpy(collider.rotationAxes, axes, sizeof(axes));
    collider.restitution = energyRetention;

    colliders.push_back(collider);
  }
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

std::vector<Vec3> SampleSDFInside(const SDFCollider &collider, float gridStep) {
  std::vector<Vec3> insidePoints;
  const float bound = CELL_SIZE * 2.0f;

  for (float x = -bound; x <= bound; x += gridStep) {
    for (float y = -bound; y <= bound; y += gridStep) {
      for (float z = -bound; z <= bound; z += gridStep) {
        Vec3 localPosition{x, y, z};
        float d = sdfDispatch(collider.type, localPosition);
        if (d < 0.0f) {
          Vec3 worldPos = collider.worldPosition +
                          collider.rotationAxes[0] * localPosition.x +
                          collider.rotationAxes[1] * localPosition.y +
                          collider.rotationAxes[2] * localPosition.z;
          insidePoints.push_back(worldPos);
        }
      }
    }
  }
  return insidePoints;
}


// Triangle Collision Benchmarking Functions

// adapted from "Real-Time Collision Detection" by Christer Ericson
Vec3 ClosestPtPointTriangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c)
{
  Vec3 ab = b - a;
  Vec3 ac = c - a;
  Vec3 bc = c - b;
  // Compute parametric position s for projection P’ of P on AB,
  // P’ = A + s*AB, s = snom/(snom+sdenom)
  float snom = (p - a).Dot(ab), sdenom = (p - b).Dot(a - b);
  // Compute parametric position t for projection P’ of P on AC,
  // P’ = A + t*AC, s = tnom/(tnom+tdenom)
  float tnom = (p - a).Dot(ac), tdenom = (p - c).Dot(a - c);
  if (snom <= 0.0f && tnom <= 0.0f) return a; // Vertex region early out
  // Compute parametric position u for projection P’ of P on BC,
  // P’ = B + u*BC, u = unom/(unom+udenom)
  float unom = (p - b).Dot(bc), udenom = (p - c).Dot(b - c);
  if (sdenom <= 0.0f && unom <= 0.0f) return b; // Vertex region early out
  if (tdenom <= 0.0f && udenom <= 0.0f) return c; // Vertex region early out
  // P is outside (or on) AB if the triple scalar product [N PA PB] <= 0
  Vec3 n = Cross(b - a, c - a);
  float vc = n.Dot(Cross(a - p, b - p));
  // If P outside AB and within feature region of AB,
  // return projection of P onto AB
  if (vc <= 0.0f && snom >= 0.0f && sdenom >= 0.0f)
    return a + ab * (snom / (snom + sdenom)) ;
  // P is outside (or on) BC if the triple scalar product [N PB PC] <= 0
  float va = n.Dot(Cross(b - p, c - p));
  // If P outside BC and within feature region of BC,
  // return projection of P onto BC
  if (va <= 0.0f && unom >= 0.0f && udenom >= 0.0f)
    return b + bc * (unom / (unom + udenom));
  // P is outside (or on) CA if the triple scalar product [N PC PA] <= 0
  float vb = n.Dot(Cross(c - p, a - p));
  // If P outside CA and within feature region of CA,
  // return projection of P onto CA
  if (vb <= 0.0f && tnom >= 0.0f && tdenom >= 0.0f)
    return a + ac * (tnom / (tnom + tdenom));
  // P must project inside face region. Compute Q using barycentric coordinates
  float u = va / (va + vb + vc);
  float v = vb / (va + vb + vc);
  float w = 1.0f - u - v; // = vc / (va + vb + vc)
  return a * u + b * v + c * w;
}

void ProjectParticleTri(const Vec3 &p, const Vec3 &velocity,
                        const TriCollider& triCollider,Vec3& closestPointOut) {
  float minDistanceSquared = std::numeric_limits<float>::infinity();
  Vec3 closestPoint = Vec3{0.0f, 0.0f, 0.0f};

  for (int i = 0; i + 2 < triCollider.triangles.size(); i += 3) {
    Vec3 q = ClosestPtPointTriangle(p, triCollider.triangles[i],
                                    triCollider.triangles[i + 1],
                                    triCollider.triangles[i + 2]);
    
    Vec3 diff = p - q;
    float distanceSquared = diff.Dot(diff);
    if (distanceSquared < minDistanceSquared) {
      minDistanceSquared = distanceSquared;
      closestPoint = q;
    }
  }
  closestPointOut = closestPoint;
}

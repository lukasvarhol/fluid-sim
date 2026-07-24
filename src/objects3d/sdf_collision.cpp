#include "sdf_collision.h"
#include <cstring>


void addCollider(SDFCollider* colliders, RGObject* objects, size_t idx,  AppState* as) {
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
#ifdef USE_CUDA
  CudaBuffers& cb = *as->cudaBuffers;
  cudaMemcpy(cb.colliders_d, colliders, sizeof(SDFCollider) * MAX_OBJECTS,
             cudaMemcpyHostToDevice);
  //printf("Memory updated too!\n");
#endif
}

void deleteCollider(SDFCollider* colliders, size_t idx, AppState *as) {
  colliders[idx] = SDFCollider{};
#ifdef USE_CUDA
  CudaBuffers& cb = *as->cudaBuffers;
  cudaMemcpy(cb.colliders_d, colliders, sizeof(SDFCollider) * MAX_OBJECTS,
             cudaMemcpyHostToDevice);
  //printf("Memory updated!\n");
#endif
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

void ProjectParticleTri(const Vec3 &p, const Vec3 &velocity,
                        const TriCollider& triCollider,Vec3& closestPointOut) {
  float minDistanceSquared = std::numeric_limits<float>::infinity();
  Vec3 closestPoint = Vec3{0.0f, 0.0f, 0.0f};

  for (size_t i = 0; i + 2 < triCollider.count; i += 3) {
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

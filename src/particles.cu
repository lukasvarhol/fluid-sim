#include "particles.cuh"

/*******************************************************************************
 *
 *
 ******************************************************************************/
__global__ void gravityPredictKernel(Vec3 *positions, Vec3 *velocities,
                                     Vec3 *predictedPositions, float gravity,
                                     float mouseStrength, float mouseRadius,
                                     Vec3 rayOrigin, Vec3 rayDir, float dt,
                                     int n) {

  int i = threadIdx.x + blockDim.x * blockIdx.x;
  if (i < n) {
    velocities[i] += Vec3{0.0f, gravity, 0.0f} * dt;

      if (mouseStrength != 0.0f) {
	Vec3  toParticle = positions[i] - rayOrigin;
	// project onto ray, then get perpendicular distance
	float along      = toParticle.Dot(rayDir);
	Vec3  closest    = rayOrigin + rayDir * along;
	Vec3  perp       = positions[i] - closest;
	float d2         = perp.Dot(perp);

	if (d2 < mouseRadius*mouseRadius && d2 > 1e-6f) {
	  float d       = std::sqrt(d2);
	  float falloff = 1.0f - (d / mouseRadius);
	  Vec3  dir     = perp * (1.0f / d);   // push/pull away from ray axis
	  velocities[i] += dir * (mouseStrength * falloff * dt);
	}
      }
    predictedPositions[i] = positions[i] + velocities[i] * dt;
  }
}

void gravityPredict(std::vector<Vec3> &positions_h,
                    std::vector<Vec3> &velocities_h,
                    std::vector<Vec3> &predictedPositions_h, float gravity,
                    float mouseStrength, float mouseRadius, Vec3 rayOrigin,
                    Vec3 rayDir, float dt, int n) {

  Vec3 *positions_d, *velocities_d, *predictedPositions_d;
  size_t size = n * sizeof(Vec3);

  cudaMalloc((void **)&positions_d, size);
  cudaMalloc((void **)&velocities_d, size);
  cudaMalloc((void **)&predictedPositions_d, size);

  cudaMemcpy(positions_d, positions_h.data(), size, cudaMemcpyHostToDevice);
  cudaMemcpy(velocities_d, velocities_h.data(), size, cudaMemcpyHostToDevice);

  gravityPredictKernel<<<ceil(n / 256.0), 256>>>(
      positions_d, velocities_d, predictedPositions_d, gravity, mouseStrength,
      mouseRadius, rayOrigin, rayDir, dt, n);

  cudaMemcpy(predictedPositions_h.data(), predictedPositions_d, size,
             cudaMemcpyDeviceToHost);

  cudaFree(positions_d);
  cudaFree(velocities_d);
  cudaFree(predictedPositions_d);
}


